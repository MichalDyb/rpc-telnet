struct MyData { /* dane wejściowe i wyjściowe) */
    opaque message<>;
    int state;
    int client_id;
};

program MY_PROGRAM { /* definicja programu RPC o nazwie MY_PROGRAM */
    version MY_VERSION { /* program składający się z jednej wersji MY_VERSION */
        MyData EXECUTE(MyData) = 1; /* numer procedury */
    } = 1; /* numer wersji */
} = 0x31230000; /* numer programu*/