while getopts :v:e flag
do
    case "${flag}" in
        v) verbose="$OPTARG";;
        :) verbose="";;
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
    actual=$(./build/ariac "$1" std/std.ar 2>&1 | sed -r "s/\x1B\[([0-9]{1,3}(;[0-9]{1,2})?)?[mGK]//g" | grep error:)

    # echo -e "expected:\n$expected"

    if [[ -z "$expected" ]]; then
        extra_actual_errors=$actual
    else
        extra_actual_errors=$(echo "$actual" | grep -v "$expected")
    fi
    # echo -e "actual:\n$extra_actual_errors"
    if [[ ! -z "$extra_actual_errors" && "$2" -eq 1 ]]; then
        echo -e "$(echo "$extra_actual_errors" | sed 's/^/- /')"
    fi

    extra_expected_errors=$(echo "$expected" | grep -v "$actual")
    if [[ ! -z "$extra_expected_errors" && "$2" -eq 1 ]]; then
        echo -e "$(echo "$extra_expected_errors" | sed 's/^/+ /')"
    fi

    if [[ ! -z "$extra_actual_errors" || ! -z "$extra_expected_errors" ]]; then
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

    if [[ "$2" -eq 1 ]]; then
        build/ariac $1 std/std.ar
    else
        build/ariac $1 std/std.ar 2>/dev/null >/dev/null
    fi
}

readonly VALID_VAL=0
readonly INVALID_VAL=1

# $1: directory: string
# $2: compile_invalid_srcfiles: bool
build_tests_in_dir() {
    for prog in `find $1 -name "*.ar" | sort`; do
        echo -n "Building $prog..."

        if [[ -z ${verbose+x} ]]; then
            local is_verbose=0
        elif [[ $verbose = "" ]] || [[ $verbose = $prog ]]; then
            local is_verbose=1
        else 
            local is_verbose=0
        fi

        if [[ "$2" -eq 1 ]]; then
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

        if [[ "$is_verbose" -eq 1 && $compilestatus -ne 0 ]]; then
            echo "$output"
            if [[ -n "$output" ]]; then
                echo ""
            fi
        fi
    done
}

if [ ! -f build/ariac ]; then
    echo "Compiler not found; testing ended prematurely"
    exit 1
fi

build_tests_in_dir "tests/valid/" 0
build_tests_in_dir "tests/invalid/" 1

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
