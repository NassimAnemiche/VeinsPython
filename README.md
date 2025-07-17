# Veins Python Integration

## Prerequisites
- OMNeT++ IDE
- SUMO (Simulation of Urban MObility)
- Veins Framework

## Setup Instructions

### 1. Import Project
1. Open OMNeT++ IDE
2. Import the `veins_python_basic` project into your workspace
3. Place `MyVeinsApp.cc` and `MyVeinsApp.h` in:
   ```
   veins/src/veins/modules/application/traci/
   ```

### 2. Launch SUMO
Open a terminal and run:
```powershell
python $env:VEINS_ROOT\bin\veins_launchd -vv -c "$env:SUMO_HOME\bin\sumo.exe"
```

### 3. Run Simulation
1. Build the project in OMNeT++
2. Right-click on the project
3. Select "Run As" -> "OMNeT++ Simulation"

## Output Messages
The simulation will show:
```
=== VEHICLE INITIALIZED ===
ID: [vehicle_id]
=========================

=== BSM RECEIVED ===
RECEIVER: [vehicle_id]
SENDER: [sender_id]
SENDER POS: (x, y)
DISTANCE: z m
===================
```

## Troubleshooting
If you encounter permission errors with result files:
1. Close OMNeT++.
2. Delete the contents of the `results` folder.
3. Restart the simulation.

If you experience issues launching the OMNeT++ simulation (e.g., the Qtenv keeps getting blocked):
1. Locate the Qtenv file in the project folder.
2. Delete the Qtenv file.
3. Relaunch the simulation.

## Note
Make sure SUMO is running before starting the OMNeT++ simulation to ensure proper vehicle communication.