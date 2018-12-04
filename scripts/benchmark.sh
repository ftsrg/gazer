#!/bin/bash

# Parse input arguments
for i in "$@"
do
case $i in
    --directory=*)
    DIRECTORY="${i#*=}"
    shift
    ;;
    --timeout=*)
    TIMEOUT_SECONDS="${i#*=}"
    shift
    ;;
    --output-dir=*)
    OUTPUT_DIR="${i#*=}"
    shift
    ;;
    --gazer-location=*)
    GAZER_LOCATION="${i#*=}"
    shift
    ;;
    --ulimit=*)
    ULIMIT_SETTING="${i#*=}"
    shift
    ;;
    --tries=*)
    TRIES="${i#*=}"
    shift
    ;;
esac
done

echo "DIRECTORY   = ${DIRECTORY}"
echo "TIMEOUT     = ${TIMEOUT_SECONDS}"
echo "OUTPUT DIR  = ${OUTPUT_DIR}"
echo "GAZER LOC   = ${GAZER_LOCATION}"
echo "ULIMIT      = ${ULIMIT_SETTING}"
echo "TRIES      = ${TRIES}"

CSVOUT="${OUTPUT_DIR}/measurements.csv"
TIMESFILE="${OUTPUT_DIR}/times.txt"
mkdir -p ${OUTPUT_DIR}
mkdir -p "${OUTPUT_DIR}/stdout"

touch $CSVOUT
echo "Input,Expected,Status,Time,Unwind,Count" > $CSVOUT

# Read the input directory
for f in $(find ${DIRECTORY} -name '*.c');
do
    C_FILE_PATH=`realpath ${f}`
    BASE=`basename ${f} .c`
    BCFILENAME="${OUTPUT_DIR}/${BASE}.bc"
    echo "${C_FILE_PATH} ${BCFILENAME}"
    
    EXPECTED_RESULT=""
    case "$BASE" in
        *false-unreach-call*)
        EXPECTED_RESULT="Fail"
        ;;
        *true-unreach-call*)
        EXPECTED_RESULT="Success"
        ;;
    esac

    # Run clang
    clang -O1 -g -Xclang -disable-llvm-passes -c -emit-llvm     \
        ${C_FILE_PATH} -o ${BCFILENAME}

    CLANG_EXIT=$?
    if [ ${CLANG_EXIT} -ne 0 ]; then
        echo "$C_FILE_PATH,$EXPECTED_RESULT,ClangError,0,0,0" >> $CSVOUT
        continue
    fi

    for i in $(seq 1 $TRIES);
    do
        for unwind in {1,8,10,20,30,40};
        do
            echo "Running ${BCFILENAME} with unwind $unwind ($i)..."
            OUTFILE="$OUTPUT_DIR/stdout/$BASE-$unwind-$i.out"
            # Run gazer on the bitcode file
            timeout "${TIMEOUT_SECONDS}s"                                   \
                /usr/bin/time -f %e $GAZER_LOCATION -bmc -optimize -inline  \
                -inline-globals -unwind $unwind -trace                      \
                ${BCFILENAME} 1> $OUTFILE 2> $TIMESFILE

            # Check if we timed out or not
            GAZER_EXIT=$?
            if [ $GAZER_EXIT -eq 124 ]; then
                echo "$C_FILE_PATH,$EXPECTED_RESULT,Timeout,0,$unwind,$i" >> $CSVOUT
                continue
            fi

            # Check the status
            if grep -q "Verification SUCCESSFUL" $OUTFILE; then
                GAZER_STATUS="Success"
            elif grep -q "Verification FAILED" $OUTFILE; then
                GAZER_STATUS="Fail"
            else
                GAZER_STATUS="Unknown"
            fi

            RUNTIME=$(tr -d '\n' < $TIMESFILE)

            echo "$C_FILE_PATH,$EXPECTED_RESULT,$GAZER_STATUS,$RUNTIME,$unwind,$i" >> $CSVOUT
        done
    done
done
