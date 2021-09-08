make clean 2>/dev/null >/dev/null
echo -n "Building the compiler..."
make build/bin/aria 2>/dev/null >/dev/null
if [[ $? -ne 0 ]]
then 
    echo "FAIL"
    echo "Compiler cannot be compiled; testing ended prematurely"
    exit
else
    echo "OK"
fi

ok_count=0
fail_count=0

for prog in `find tests/valid/ -name "*.ar"`; do
    echo -n "Building $prog..."
    build/bin/aria $prog 2>/dev/null >/dev/null
    if [[ $? -ne 0 ]]
    then
        echo "FAIL"
        ((fail_count++))
    else 
        echo "OK"
        ((ok_count++))
    fi
done

for prog in `find tests/invalid/ -name "*.ar"`; do
    echo -n "Building $prog..."
    build/bin/aria $prog 2>/dev/null >/dev/null
    if [[ $? -ne 1 ]]
    then
        echo "FAIL"
        ((fail_count++))
    else 
        echo "OK"
        ((ok_count++))
    fi
done

echo "$ok_count ok; $fail_count failed"
