
def parse_command_line(args, cfg):
    i = 1
    while i < len(args):
        for entry in cfg:
            if args[i] == '--'+entry:
                if type(cfg[entry]) is str or cfg[entry] == None:
                    cfg[entry] = args[i + 1]
                elif type(cfg[entry]) is list:
                    l = []
                    while (i + 1) < len(args) and args[i + 1].isnumeric():
                        l.append(int(args[i + 1]))
                        i += 1
                    cfg[entry] = l
                elif type(cfg[entry]) is int:
                    cfg[entry] = int(args[i + 1])
                i += 1
        i += 1
    return cfg
