cat dump.txt | grep m3 | awk -F '.' '{print $1}' | awk -F '-' '{print $2}' > nodeids
cat dump.txt | grep m3 | awk '{print $2}' > hexnodes
paste nodeids hexnodes > node_id_addr.txt
rm nodeids
rm hexnodes
