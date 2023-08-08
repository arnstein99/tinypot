#
function main
{
    my_pid=$$
    delay=$1
    shift
    while true
    do
        array=$(./randlist ${@})
        tinypot - ${array[@]} &
        ident=$!
        echo "PIDs are ${my_pid} ${ident}" >&2
        trap "cleanup $ident" \
            SIGHUP SIGINT SIGQUIT SIGABRT SIGKILL SIGUSR1 SIGUSR2 SIGTERM
        sleep $delay
        kill -9 $ident
    done
}
function cleanup
# process_id
{
    echo "Killing process $1" >&2
    kill -9 "$1"
    exit 0
}
main "$@"
