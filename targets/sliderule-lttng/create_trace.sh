OPTION=$1

lttng create sliderule-session --output=/tmp/sliderule-session

echo "Enabling userspace trace points"
lttng enable-event --userspace sliderule:start
lttng enable-event --userspace sliderule:stop
lttng enable-event --userspace lttng_ust_pthread:*
lttng enable-event --userspace lttng_ust_libc:*

if [ $# -gt 0 ]; then
if [ $OPTION = "kernel" ]; then
    echo "Enabling kernel trace points"
    lttng enable-event -k   sched_switch,sched_waking,sched_pi_setprio,sched_process_fork,sched_process_exit,sched_process_free,sched_wakeup,irq_softirq_entry,irq_softirq_raise,irq_softirq_exit,irq_handler_entry,irq_handler_exit,lttng_statedump_process_state,lttng_statedump_start,lttng_statedump_end,lttng_statedump_network_interface,lttng_statedump_block_device,block_rq_complete,block_rq_insert,block_rq_issue,block_bio_frontmerge,sched_migrate,sched_migrate_task,power_cpu_frequency,net_dev_queue,netif_receive_skb,net_if_receive_skb,timer_hrtimer_start,timer_hrtimer_cancel,timer_hrtimer_expire_entry,timer_hrtimer_expire_exit
    lttng enable-event -k --syscall --all
fi
fi

lttng start