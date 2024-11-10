#!/bin/bash

# # Path to the hd0.32k file
# header_file="hd0.32k"

# Path to dcraw executable
dcraw_path="./bin/dcraw"

# Default paths
default_raw_path="/dev/shm"
default_output_path="."

# Function to process each file
process_file() {
    raw_file="$1"
    output_path="$2"
    base_name=$(basename "$raw_file")
    output_file="${base_name}"
    temp_output_file="${raw_file%.*}.ppm" # Assuming dcraw creates .ppm files
    final_output_file="$output_path/$base_name.ppm"

    # # Combine header and raw data
    # sudo cat "$header_file" "$raw_file" > "$output_file"

    # Run dcraw on the combined file
    # "$dcraw_path" "$output_file"
    "$dcraw_path" "$raw_file"

    # Move the output file to the output path
    if [ -f "$temp_output_file" ]; then
        mv "$temp_output_file" "$final_output_file"
    else
        echo "Warning: Output file not found for $raw_file"
    fi

    # Clean up
    rm -f "$raw_file"
}

# Export the function and variables so they are available to subshells
export -f process_file
export header_file
export dcraw_path

# Set input and output paths with default values
raw_path="${1:-$default_raw_path}"
output_path="${2:-$default_output_path}"

# Validate directories
if [ ! -d "$raw_path" ]; then
    echo "Error: Input folder '$raw_path' does not exist."
    exit 1
fi

if [ ! -d "$output_path" ]; then
    echo "Error: Output folder '$output_path' does not exist."
    exit 1
fi

# Display configuration
echo "Processing files from '$raw_path' to '$output_path'."

# Use find and xargs to process files in parallel
find "$raw_path" -name 'out.*.raw' | xargs -n 1 -P 3 -I {} bash -c 'process_file "$@"' _ {} "$output_path"