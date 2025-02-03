#!/bin/bash
# Server info
host="localhost"
port=36689  # Replace with your desired port

# Create a new tmux session
tmux new-session -d -s client_session

# Loop through 500 clients
for i in {1..999}
do
    # Create a new tmux window for each client and run the client command
    tmux new-window -t client_session: -n "handle$i" "./cclient handle$i $host $port"
    echo "Started client with handle$i in tmux window handle$i"
done

# Attach to the tmux session to view the windows
tmux attach -t client_session