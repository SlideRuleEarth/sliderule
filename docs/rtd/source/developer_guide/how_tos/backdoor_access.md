# Backdoor Access

2023-09-20

## Query the Logs

1. Connect to `{organization}-monitor` via AWS session manager
2. At prompt `sudo su -` to become root
3. Query logs using the Loki command line client: `logcli`

Example Pattern Match
```bash
logcli query -o raw --limit 1000000 --batch 5000 '{job="sliderule",level=~"debug|info|warning|error|critical"} |= "My specific error message I am looking for"'
```

Example Date Ramge
```bash
logcli query -o raw --limit 1000000 --batch 5000 --from="2023-09-20T09:00:00Z" --to "2023-09-20T10:00:00Z" '{job="sliderule",level=~"debug|info|warning|error|critical"}'
```

Example Dump of Critical Messages (if today was 2023-09-20)
```bash
logcli query -o raw --limit 1000000 --batch 5000 --from="2023-09-16T00:00:00Z" '{job="sliderule",level=~"critical"}'
```

## Check Container Health

1. Connect to the desired `{organization}-node` via AWS session manager
2. At prompt `sudo su -` to become root

Example Listing of Running Containers
```bash
docker ps
```

Example SlideRule Lua Prompt Access
```bash
docker attach cluster-sliderule-1
```

Example SSH Access
```bash
docker exec -it cluster-sliderule-1 bash
```
