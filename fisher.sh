#
function main
{
    my_pid=$$
    delay=$1
    shift
    while true
    do
        array=$(randlist ${@})
        service_tinypot - ${array[@]} &
        tinypot_pid=$!
        echo "My pid is ${my_pid}" >&2
        sleep $delay &
        sleep_pid="$!"
        trap "cleanup $tinypot_pid $sleep_pid" \
            SIGHUP SIGINT SIGQUIT SIGABRT SIGUSR1 SIGUSR2 SIGTERM
        wait
        kill -9 $tinypot_pid
        sleep 5  # trying to avoid "port already in use" failure
    done
}
function cleanup
# process_id process_id ...
{
    echo "Killing processes $@" >&2
    kill -9 "$@"
    exit 0
}
main "$@"
