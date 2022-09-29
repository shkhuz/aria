while getopts :v:nh flag
do
    case "${flag}" in
        h) echo "test.sh: Test framework for the Aria compiler"
           echo "Usage: ./test.sh [options]"
           echo ""
           echo "Options:"
           echo "  -v <file>                   Print additional info for <file>]"
           echo "      (set by default for failed tests)"
           echo "      If * is passed as an argument, ie. if <file> is *, "
           echo "      then -v is set for all the test files."
           echo "  -n                          Suppress additional output for all files"
           echo "      As -v is set for failed tests by default, this option"
           echo "      is used to suppress all additional output from stderr."
           echo "  -h                          Display this help and exit"
           echo ""
           echo "To report bugs, please see:"
           echo "<https://github.com/shkhuz/aria/issues/>"
           exit 0;;
        n) suppress=1;;
        v) verbose="$OPTARG";;
        :) echo "Argument required for -$OPTARG; exiting" >&2; exit 1;;
        *) echo "Invalid command line flags; exiting" >&2; exit 1;;
    esac
done

red_color="\x1b[31m"
bold_red_color="\x1b[1;31m"
green_color="\x1B[32m"
bold_green_color="\x1B[1;32m"
reset_color="\x1b[0m"

print_ok() {
    printf "${green_color}OK${reset_color}\n"
}

print_fail() {
    printf "${red_color}FAIL${reset_color}\n"
}

ok_count=0
fail_count=0

# $1: srcfile: string
# $2: verbose: bool
compile_invalid_srcfile() {
    if [[ ! -f $1 ]]; then
        echo "compile_invalid_srcfile(): $1 not found"
        return 1
    fi

    expected=$(grep '//!' $1 | sed 's/^\/\/! //')
    buildoutput=$(./build/ariac "$1" 2>&1)
    buildstatus=$?
    actual=$(echo "$buildoutput" | sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,2})?)?[mGK]//g" | sed 's/^[^:]*://' | grep error:)

    # echo -e "expected:\n$expected\nactual:\n$actual"

    if [[ "$buildstatus" -eq 0 ]]; then
        if [[ "$2" -ne 0 ]]; then
            echo -e "  Compiler exited with $buildstatus, but build should fail"
        fi
        return 1
    fi

    if [[ -z "$expected" ]]; then
        extra_actual_errors=$actual
    else
        extra_actual_errors=$(echo "$actual" | grep -xvF "$expected")
    fi

    if [[ ! -z "$extra_actual_errors" && "$2" -ne 0 ]]; then
        echo -e "$(echo "$extra_actual_errors" | sed 's/^/+ /')"
    fi

    if [[ -z "$actual" ]]; then
        extra_expected_errors=$expected
    else 
        extra_expected_errors=$(echo "$expected" | grep -xvF "$actual")
    fi
    if [[ ! -z "$extra_expected_errors" && "$2" -ne 0 ]]; then
        echo -e "$(echo "$extra_expected_errors" | sed 's/^/- /')"
    fi

    if [[ ! -z "$extra_actual_errors" || ! -z "$extra_expected_errors" ]]; then
        return 1
    fi
    return 0
}

# $1: srcfile: string
# $2: verbose: bool
compile_valid_srcfile_and_run_exec() {
    compile_valid_srcfile $@
    if [[ $? -ne 0 ]]; then
        return 1
    fi
    output=$(./a.out 2>&1)
    execexitcode=$?
    if [[ $2 -ne 0 ]]; then
        echo "$output"
    fi
    
    if [[ $execexitcode -ne 0 ]]; then
        if [[ $2 -ne 0 ]]; then
            echo "  Executable returned $execexitcode, but it should return 0"
        fi
        return 1
    fi
    return 0
}

# $1: srcfile: string
# $2: verbose: bool
compile_valid_srcfile() {
    if [[ ! -f $1 ]]; then
        echo "compile_valid_srcfile(): $1 not found"
        return 1
    fi

    if [[ $2 -ne 0 ]]; then
        build/ariac $1 2>&1
    else
        build/ariac $1 2>/dev/null >/dev/null
    fi
    buildstatus=$?

    if [[ "$buildstatus" -ne 0 ]]; then
        if [[ "$2" -ne 0 ]]; then
            echo -e "  Compiler exited with $buildstatus, but build should succeed"
        fi
        return 1
    fi
}

# $1: directory: string
# $2: test_type: int
build_tests_in_dir() {
    for prog in `find $1 -name "*.ar" | sort`; do
        if [[ $suppress -eq 1 ]]; then
            local is_verbose=0
        elif [[ -z ${verbose+x} ]]; then
            local is_verbose=1
        elif [[ $verbose = "" ]]; then
            local is_verbose=1
        elif [[ $verbose = "*" || $verbose = $prog ]]; then
            local is_verbose=2
        else 
            # this case is not mixed with the first case
            # this is deliberate
            local is_verbose=0
        fi
        
        echo -ne "Building $prog..."

        if [[ "$2" -eq 2 ]]; then
            output=$(compile_valid_srcfile_and_run_exec $prog $is_verbose)
        elif [[ "$2" -eq 1 ]]; then
            output=$(compile_invalid_srcfile $prog $is_verbose)
        elif [[ "$2" -eq 0 ]]; then
            output=$(compile_valid_srcfile $prog $is_verbose)
        fi
        compilestatus=$?

        if [[ $compilestatus -eq 0 ]]; then
            print_ok
            ((ok_count++))
        else 
            print_fail
            ((fail_count++))
        fi

        if [[ ( "$is_verbose" -eq 1 && $compilestatus -ne 0 ) || "$is_verbose" -eq 2 ]]; then
            echo "$output"
            if [[ -n "$output" ]]; then
                echo ""
            fi
        fi
    done
}

COMPILE_VALID=0
COMPILE_INVALID=1
COMPILE_VALID_AND_RUN=2

if [ ! -f build/ariac ]; then
    echo "Compiler not found; testing ended prematurely"
    exit 1
fi

build_tests_in_dir "tests/valid" $COMPILE_VALID
build_tests_in_dir "tests/invalid/" $COMPILE_INVALID
build_tests_in_dir "tests/cg" $COMPILE_VALID_AND_RUN

if [[ $fail_count -ne 0 ]]
then
    errcolor=$bold_red_color
else 
    successcolor=$bold_green_color
fi

printf "${successcolor}$ok_count ok${reset_color}; ${errcolor}${fail_count} failed${reset_color}\n"

if [[ $fail_count -ne 0 ]]
then
    exit 1
fi
