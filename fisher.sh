#
function main
{
    case $# in
    5|6)
        ;;
    *)
        echo "Usage: fisher.sh [address|-] lifespan first_port last_port port_count [prohibited_file]" >&2
        exit 1
        ;;
    esac

    my_pid=$$
    address="$1"
    shift
    delay="$1"
    shift
    while true
    do
        array=$(randlist ${@})
        service_tinypot "$address" ${array[@]} &
        tinypot_pid=$!
        echo "My pid is ${my_pid}" >&2
        sleep $delay &
        sleep_pid="$!"
        trap "cleanup $tinypot_pid $sleep_pid" SIGINT SIGQUIT SIGABRT SIGTERM
        trap "cycle                $sleep_pid" SIGHUP
        wait "$sleep_pid" >/dev/null 2>&1
        kill -9 $tinypot_pid
        sleep 5  # trying to avoid "port already in use" failure
    done
}

function cycle
# process_id process_id ...
{
    kill -9 "$@"
}

function cleanup
# process_id process_id ...
{
    cycle "$@"
    echo "fisher.sh terminated by signal" >&2
    exit 1
}

main "$@"
