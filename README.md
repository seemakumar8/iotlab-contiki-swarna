# SWARNA

- border-router: Contains the source code that runs on the border router (gateway node) of the IoT LAB testbed. This node acts as a root node in the network.
- relay-node: Contains the source code for relay nodes. Relay nodes do not generate data and instead only forwards the control and data traffic towards the root.
- client-node: Generates the data which is fowarded towards the root node. 
- verifier: Contains the source code for the remote verifier which runs on a Linux based system. Verifier generates the attestation requests and verifies the responses.
