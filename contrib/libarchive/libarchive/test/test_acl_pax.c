/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD: head/contrib/libarchive/libarchive/test/test_acl_pax.c 248616 2013-03-22 13:36:03Z mm $");

/*
 * Exercise the system-independent portion of the ACL support.
 * Check that pax archive can save and restore ACL data.
 *
 * This should work on all systems, regardless of whether local
 * filesystems support ACLs or not.
 */

static unsigned char buff[16384];

struct acl_t {
	int type;  /* Type of ACL: "access" or "default" */
	int permset; /* Permissions for this class of users. */
	int tag; /* Owner, User, Owning group, group, other, etc. */
	int qual; /* GID or UID of user/group, depending on tag. */
	const char *name; /* Name of user/group, depending on tag. */
};

static struct acl_t acls0[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, 0, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, 0, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_WRITE,
	  ARCHIVE_ENTRY_ACL_OTHER, 0, "" },
};

static struct acl_t acls1[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_WRITE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
};

static struct acl_t acls2[] = {
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_EXECUTE | ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_USER, 77, "user77" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0,
	  ARCHIVE_ENTRY_ACL_USER, 78, "user78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_READ,
	  ARCHIVE_ENTRY_ACL_GROUP_OBJ, -1, "" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, 0007,
	  ARCHIVE_ENTRY_ACL_GROUP, 78, "group78" },
	{ ARCHIVE_ENTRY_ACL_TYPE_ACCESS, ARCHIVE_ENTRY_ACL_WRITE | ARCHIVE_ENTRY_ACL_EXECUTE,
	  ARCHIVE_ENTRY_ACL_OTHER, -1, "" },
};

static void
set_acls(struct archive_entry *ae, struct acl_t *acls, int n)
{
	int i;

	archive_entry_acl_clear(ae);
	for (i = 0; i < n; i++) {
		archive_entry_acl_add_entry(ae,
		    acls[i].type, acls[i].permset, acls[i].tag, acls[i].qual,
		    acls[i].name);
	}
}

static int
acl_match(struct acl_t *acl, int type, int permset, int tag, int qual, const char *name)
{
	if (type != acl->type)
		return (0);
	if (permset != acl->permset)
		return (0);
	if (tag != acl->tag)
		return (0);
	if (tag == ARCHIVE_ENTRY_ACL_USER_OBJ)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_GROUP_OBJ)
		return (1);
	if (tag == ARCHIVE_ENTRY_ACL_OTHER)
		return (1);
	if (qual != acl->qual)
		return (0);
	if (name == NULL)
		return (acl->name == NULL || acl->name[0] == '\0');
	if (acl->name == NULL)
		return (name == NULL || name[0] == '\0');
	return (0 == strcmp(name, acl->name));
}

static void
compare_acls(struct archive_entry *ae, struct acl_t *acls, int n, int mode)
{
	int *marker = malloc(sizeof(marker[0]) * n);
	int i;
	int r;
	int type, permset, tag, qual;
	int matched;
	const char *name;

	for (i = 0; i < n; i++)
		marker[i] = i;

	while (0 == (r = archive_entry_acl_next(ae,
			 ARCHIVE_ENTRY_ACL_TYPE_ACCESS,
			 &type, &permset, &tag, &qual, &name))) {
		for (i = 0, matched = 0; i < n && !matched; i++) {
			if (acl_match(&acls[marker[i]], type, permset,
				tag, qual, name)) {
				/* We found a match; remove it. */
				marker[i] = marker[n - 1];
				n--;
				matched = 1;
			}
		}
		if (tag == ARCHIVE_ENTRY_ACL_USER_OBJ) {
			if (!matched) printf("No match for user_obj perm\n");
			failure("USER_OBJ permset (%02o) != user mode (%02o)",
			    permset, 07 & (mode >> 6));
			assert((permset << 6) == (mode & 0700));
		} else if (tag == ARCHIVE_ENTRY_ACL_GROUP_OBJ) {
			if (!matched) printf("No match for group_obj perm\n");
			failure("GROUP_OBJ permset %02o != group mode %02o",
			    permset, 07 & (mode >> 3));
			assert((permset << 3) == (mode & 0070));
		} else if (tag == ARCHIVE_ENTRY_ACL_OTHER) {
			if (!matched) printf("No match for other perm\n");
			failure("OTHER permset (%02o) != other mode (%02o)",
			    permset, mode & 07);
			assert((permset << 0) == (mode & 0007));
		} else {
			failure("Could not find match for ACL "
			    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
			    type, permset, tag, qual, name);
			assert(matched == 1);
		}
	}
	assertEqualInt(ARCHIVE_EOF, r);
	assert((mode_t)(mode & 0777) == (archive_entry_mode(ae) & 0777));
	failure("Could not find match for ACL "
	    "(type=%d,permset=%d,tag=%d,qual=%d,name=``%s'')",
	    acls[marker[0]].type, acls[marker[0]].permset,
	    acls[marker[0]].tag, acls[marker[0]].qual, acls[marker[0]].name);
	assert(n == 0); /* Number of ACLs not matched should == 0 */
	free(marker);
}

DEFINE_TEST(test_acl_pax)
{
	struct archive *a;
	struct archive_entry *ae;
	size_t used;
	FILE *f;
	void *reference;
	size_t reference_size;

	/* Write an archive to memory. */
	assert(NULL != (a = archive_write_new()));
	assertA(0 == archive_write_set_format_pax(a));
	assertA(0 == archive_write_add_filter_none(a));
	assertA(0 == archive_write_set_bytes_per_block(a, 1));
	assertA(0 == archive_write_set_bytes_in_last_block(a, 1));
	assertA(0 == archive_write_open_memory(a, buff, sizeof(buff), &used));

	/* Write a series of files to the archive with different ACL info. */

	/* Create a simple archive_entry. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file");
        archive_entry_set_mode(ae, S_IFREG | 0777);

	/* Basic owner/owning group should just update mode bits. */
	set_acls(ae, acls0, sizeof(acls0)/sizeof(acls0[0]));
	assertA(0 == archive_write_header(a, ae));

	/* With any extended ACL entry, we should read back a full set. */
	set_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]));
	assertA(0 == archive_write_header(a, ae));


	/* A more extensive set of ACLs. */
	set_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]));
	assertA(0 == archive_write_header(a, ae));

	/*
	 * Check that clearing ACLs gets rid of them all by repeating
	 * the first test.
	 */
	set_acls(ae, acls0, sizeof(acls0)/sizeof(acls0[0]));
	assertA(0 == archive_write_header(a, ae));
	archive_entry_free(ae);

	/* Close out the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_write_close(a));
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Write out the data we generated to a file for manual inspection. */
	assert(NULL != (f = fopen("testout", "wb")));
	assertEqualInt(used, (size_t)fwrite(buff, 1, (unsigned int)used, f));
	fclose(f);

	/* Write out the reference data to a file for manual inspection. */
	extract_reference_file("test_acl_pax.tar");
	reference = slurpfile(&reference_size, "test_acl_pax.tar");

	/* Assert that the generated data matches the built-in reference data.*/
	failure("Generated pax archive does not match reference; compare 'testout' to 'test_acl_pax.tar' reference file.");
	assertEqualMem(buff, reference, reference_size);
	failure("Generated pax archive does not match reference; compare 'testout' to 'test_acl_pax.tar' reference file.");
	assertEqualInt((int)used, reference_size);
	free(reference);

	/* Read back each entry and check that the ACL data is right. */
	assert(NULL != (a = archive_read_new()));
	assertA(0 == archive_read_support_format_all(a));
	assertA(0 == archive_read_support_filter_all(a));
	assertA(0 == archive_read_open_memory(a, buff, used));

	/* First item has no ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	failure("Basic ACLs shouldn't be stored as extended ACLs");
	assert(0 == archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

	/* Second item has a few ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	failure("One extended ACL should flag all ACLs to be returned.");
	assert(4 == archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls1, sizeof(acls1)/sizeof(acls1[0]), 0142);
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

	/* Third item has pretty extensive ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	assertEqualInt(6, archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	compare_acls(ae, acls2, sizeof(acls2)/sizeof(acls2[0]), 0543);
	failure("Basic ACLs should set mode to 0543, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0543);

	/* Fourth item has no ACLs */
	assertA(0 == archive_read_next_header(a, &ae));
	failure("Basic ACLs shouldn't be stored as extended ACLs");
	assert(0 == archive_entry_acl_reset(ae, ARCHIVE_ENTRY_ACL_TYPE_ACCESS));
	failure("Basic ACLs should set mode to 0142, not %04o",
	    archive_entry_mode(ae)&0777);
	assert((archive_entry_mode(ae) & 0777) == 0142);

	/* Close the archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_close(a));
	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}
