build/[debug|release]/classify_coastnet \
        --class=40 \
        --aspect-ratio=4 \
        --network-filename=./models/coastnet-bathy-manual.pt \
        < input_filename.csv > output_filename.csv