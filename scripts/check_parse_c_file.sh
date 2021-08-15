if [[ -n $(grep "\(\<msg::error\>\|\<verror_token\>\|\<error_node\>\|\<verror_node\>\)" src/parse.cpp) ]]; then
    echo "$0: check failed"; 
    exit 1;
fi
