#!/bin/bash

# Path to the hd0.32k file
header_file="hd0.32k"

# Path to dcraw executable
dcraw_path="./bin/dcraw"

# Function to process each file
process_file() {
    raw_file="$1"
    base_name=$(basename "$raw_file")
    output_file="./${base_name}"

    # # Combine header and raw data
    # sudo cat "$header_file" "$raw_file" > "$output_file"

    # Run dcraw on the combined file
    # "$dcraw_path" "$output_file"
    "$dcraw_path" "$raw_file"


    # Clean up
    # rm -f "$raw_file" "$output_file"
    rm -f "$raw_file"
}

# Export the function and variables so they are available to subshells
export -f process_file
export header_file
export dcraw_path

# Use find to get all matching files and process them with xargs in parallel
find /dev/shm -name 'out.*.raw' | xargs -n 1 -P 3 -I {} bash -c 'process_file "$@"' _ {}
