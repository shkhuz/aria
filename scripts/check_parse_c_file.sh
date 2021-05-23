if [[ -n $(grep "\(\<error_token\>\|\<verror_token\>\|\<error_node\>\|\<verror_node\>\)" src/parse.c) ]]; then
    echo "$0: check failed"; 
    exit 1;
fi
