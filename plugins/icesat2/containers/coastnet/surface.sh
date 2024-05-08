build/[debug|release]/classify_coastnet \
        --class=41 \
        --aspect-ratio=4 \
        --network-filename=./models/coastnet-surface-manual.pt \
        < input_filename.csv > output_filename.csv
