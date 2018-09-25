#Refer: http://www.linuxfoundation.org/collaborate/workgroups/networking/netem#Delaying_only_some_traffic
#Refer: http://www.bomisofmab.com/blog/?p=100
#Refer: http://drija.com/linux/41983/simulating-a-low-bandwidth-high-latency-network-connection-on-linux/
#Setup the rate control and delay
sudo tc qdisc add dev lo root handle 1: htb default 12
sudo tc class add dev lo parent 1:1 classid 1:12 htb rate 56kbps ceil 128kbps
sudo tc qdisc add dev lo parent 1:12 netem delay 200ms

#Remove the rate control/delay
sudo tc qdisc del dev lo root

#To see what is configured on an interface, do this
sudo tc -s qdisc ls dev lo

#Replace lo with eth0/wlan0 to limit speed from wide lan