#!/bin/bash 

. ./functions

count=L1-dcache-load-misses,L1-dcache-loads,cache-misses,cache-references

vsizes=(2048  4096  8192  16384 32768 65536)
viters=(10000 10000 10000 1000  1000  1000)

csizes=(1536 24576)
asizes=(409600)
gsizes=(122880)

slowdowns=()
missrates=()

if [[ $1 == 1 ]]; then
	EnableCAT 1 0x7ff 0x800 &> /dev/null
else
	EnableCAT 0 0xffff &> /dev/null
fi

index=0
for vsize in ${vsizes[@]}; do
	slowdowns+=(); slowdowns[$index]+="$vsize, "
	missrates+=(); missrates[$index]+="$vsize, "
	
	echo "MlpRead($vsize) Victim"
	echo "Corun, Bandwidth, Latency, Slowdown, L1_miss, L1_loads, L1_missrate, LLC_miss, LLC_access, LLC_missrate"
	
	MlpReadVictimSolo $((vsize/12)) ${viters[$index]}
	slowdowns[$index]+="1.00, "
	missrates[$index]+="$l2missrate, "
	
	for csize in ${csizes[@]}; do
		BwWriteCorun $csize
		sleep 1
		MlpReadVictimCorun $((vsize/12)) ${viters[$index]}
		slowdowns[$index]+="$slowdown, "
		missrates[$index]+="$l2missrate, "
		killall $corun
        
        MlpWriteCorun $((csize/12))
		sleep 1
		MlpReadVictimCorun $((vsize/12)) ${viters[$index]}
		slowdowns[$index]+="$slowdown, "
		missrates[$index]+="$l2missrate, "
		killall $corun
	done
    
    for asize in ${asizes[@]}; do
        MemAwareWriteCorun $asize
		sleep 1
		MlpReadVictimCorun $((vsize/12)) ${viters[$index]}
		slowdowns[$index]+="$slowdown, "
		missrates[$index]+="$l2missrate, "
		killall $corun
    done
	
	for gsize in ${gsizes[@]}; do
		GpuWriteCorun 3 $gsize
		sleep 1
		MlpReadVictimCorun $((vsize/12)) ${viters[$index]}
		slowdowns[$index]+="$slowdown "
		missrates[$index]+="$l2missrate "
		killall $corun
	done
	
	index=$((index+1))
	echo ""
done

slowdown_file="figs/GTCOS-DoS-MLP-Slowdown.dat"
missrate_file="figs/GTCOS-DoS-MLP-LLCMiss.dat"
> $slowdown_file
> $missrate_file

echo "Slowdown Gnuplot:"
echo "WSS (KB), Solo, BwWrite(LLC), MlpWrite(LLC), BwWrite(DRAM), MlpWrite(DRAM), MemWrite(DRAM), GpuWrite(DRAM)"
for ((i=0;i<${#slowdowns[@]}; i++)); do
	echo ${slowdowns[$i]} | tee -a $slowdown_file
done
echo ""

echo "Missrate Gnuplot:"
echo "WSS (KB), Solo, BwWrite(LLC), MlpWrite(LLC), BwWrite(DRAM), MlpWrite(DRAM), MemWrite(DRAM), GpuWrite(DRAM)"
for ((i=0;i<${#missrates[@]}; i++)); do
	echo ${missrates[$i]} | tee -a missrate_file
done

cd figs
gnuplot bw-gen.gp
cd ..
