# $FreeBSD: head/tools/tools/mtxstat/mtxratio.sh 132679 2004-07-27 00:36:53Z jmg $
sysctl debug.mutex.prof.stats | awk '$1 ~ /[0-9]+/ { if ($3 != 0) { hld_prc = $5 / $3 * 100; lck_prc = $6 / $3 * 100 } else { hld_prc = 0; lck_prc = 0 } print $1 " " $2 " " $3 " " $4 " " $5 " " hld_prc " " $6 " " lck_prc " " substr($0, index($0, $7)); next } { print }'
