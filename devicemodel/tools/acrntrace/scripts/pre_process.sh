#!/bin/bash

IFILE=$1
STR=$2
BAK_FILE=${IFILE}.orig

if [ ! -f $BAK_FILE ]; then
	cp $IFILE $BAK_FILE
fi

echo $IFILE, $BAK_FILE, $STR

FIRST_LINE=$(grep -n $STR -m 1 $IFILE | awk -F: '{print $1}')
LAST_LINE=$(grep -n $STR $IFILE | tail -1 | awk -F: '{print $1}')
TOTAL_LINE=$(wc -l $IFILE | awk '{print $1}')

echo $FIRST_LINE, $LAST_LINE, $TOTAL_LINE

if [[ $FIRST_LINE -ge $LAST_LINE || $LAST_LINE -gt $TOTAL_LINE ]]; then
	exit 1
fi

if [[ $LAST_LINE -lt $TOTAL_LINE ]]; then
	let LAST_LINE=$((LAST_LINE))+1
	echo "sed -i ${LAST_LINE},${TOTAL_LINE}d $IFILE"
	sed -i ${LAST_LINE}','${TOTAL_LINE}'d' $IFILE
fi

if [[ $FIRST_LINE -gt 2 ]]; then
	let FIRST_LINE=$((FIRST_LINE))-1
	echo "sed -i 2,${FIRST_LINE}d $IFILE"
	sed -i '2,'${FIRST_LINE}'d' $IFILE
fi
