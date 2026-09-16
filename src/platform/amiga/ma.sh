clear

#CPUS="020 030 040 060"
CPUS="020"

for CPU in $CPUS; do
    echo "Building CPU=$CPU"

    make clean CPU=$CPU
    make all -j12 CPU=$CPU || { echo "BUILD FAILED for CPU=$CPU"; exit 1; }

    #cp OpenLara.$CPU OpenLara_${CPU}_rtg
    #cp OpenLara.$CPU OpenLara_${CPU}_rtg_nearest
    #cp OpenLara.$CPU OpenLara_${CPU}_rtg_s2x
    #cp OpenLara.$CPU OpenLara_${CPU}_lloyd
    #cp OpenLara.$CPU OpenLara_${CPU}_lloyd3d
    #cp OpenLara.$CPU OpenLara_${CPU}_wu
done

echo "All CPU builds done"
