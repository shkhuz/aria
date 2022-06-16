while getopts :v:e flag
do
    case "${flag}" in
        v) verbose="$OPTARG";;
        e) existing_compiler=1;;
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

if [[ $existing_compiler -ne 1 ]] 
then
    make clean 2>/dev/null >/dev/null
    echo -n "Building the compiler..."
    make build/ariac 2>/dev/null >/dev/null
    if [[ $? -ne 0 ]]
    then 
        print_fail
        echo "Compiler cannot be compiled; testing ended prematurely"
        exit
    else
        print_ok
    fi
fi

ok_count=0
fail_count=0

compile_file_no_verbose() {
    build/ariac $1 std/std.ar 2>/dev/null >/dev/null
}

readonly VALID_VAL=0
readonly INVALID_VAL=1

build_tests_in_dir() {
    for prog in `find $1 -name "*.ar" | sort`; do
        echo -n "Building $prog..."

        expect_errc=`sed -n -e 's/\/\/\s*errc:\s*//p' $prog`
        if [[ $expect_errc == "" ]]; then
            expect_errc=1
        fi

        if [[ -z ${verbose+x} ]]; then
            compile_file_no_verbose $prog
        else 
            if [[ $verbose = "" ]] || [[ $verbose = $prog ]]; then
                echo ""
                (set -x; build/ariac $prog std/std.ar >/dev/null)
            else 
                compile_file_no_verbose $prog
            fi
        fi

        result=$?
        if [[ $2 -eq $VALID_VAL ]]; then
            expect_errc=0
        fi

        if [[ $result -eq $expect_errc ]]; then
            print_ok
            ((ok_count++))
        else 
            print_fail
            ((fail_count++))
        fi
    done
}

if [[ ! -f build/ariac ]]; then
    echo "Compiler not found; testing ended prematurely"
    exit 1
fi

build_tests_in_dir "tests/valid/" $VALID_VAL
build_tests_in_dir "tests/invalid/" $INVALID_VAL

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
