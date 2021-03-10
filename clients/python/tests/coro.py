# python
import sys
import h5coro

###############################################################################
# DATA
###############################################################################

# set resource
resource = "file:///data/ATLAS/ATL06_20200714160647_02950802_003_01.h5"

# expected single read
h_li_exp_1 = [3432.17578125, 3438.776611328125, 3451.01123046875, 3462.688232421875, 3473.559326171875]

# expected parallel read
h_li_exp_2 = {  '/gt1l/land_ice_segments/h_li': [3432.17578125, 3438.776611328125, 3451.01123046875, 3462.688232421875, 3473.559326171875], 
                '/gt2l/land_ice_segments/h_li': [3263.659912109375, 3258.362548828125, 3.4028234663852886e+38, 3233.031494140625, 3235.200927734375],
                '/gt3l/land_ice_segments/h_li': [3043.489013671875, 3187.576171875, 3.4028234663852886e+38, 4205.04248046875, 2924.724365234375]} 


###############################################################################
# UTILITY FUNCTIONS
###############################################################################

def check_results(act, exp):
    if type(exp) == dict:
        for dataset in exp:
            for i in range(len(exp[dataset])):
                if exp[dataset][i] != act[dataset][i]:
                    print("Failed parallel read test")
                    return False
        print("Passed parallel read test")
        return True
    else:
        for i in range(len(exp)):
            if exp[i] != act[i]:
                print("Failed single read test")
                return False
        print("Passed single read test")
        return True

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Open H5Coro File #
    h5file = h5coro.file(resource)

    # Perform Single Read #
    h_li_1 = h5file.read("/gt1l/land_ice_segments/h_li", 0, 19, 5)
    check_results(h_li_1, h_li_exp_1)

    # Perform Parallel Read #
    datasets = [["/gt1l/land_ice_segments/h_li", 0, 19, 5],
                ["/gt2l/land_ice_segments/h_li", 0, 19, 5],
                ["/gt3l/land_ice_segments/h_li", 0, 19, 5]]
    h_li_2 = h5file.readp(datasets)
    check_results(h_li_2, h_li_exp_2)
