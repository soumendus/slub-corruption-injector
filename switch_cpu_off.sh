#for x in /sys/devices/system/cpu/cpu[1-9]*/online; do
#  echo 1 >"$x"
#done

for i in {1,2,3,4,5,6,7,8,9,10,11}
do
	#echo /sys/devices/system/cpu/cpu$i/online
	echo 1 > /sys/devices/system/cpu/cpu$i/online
done

