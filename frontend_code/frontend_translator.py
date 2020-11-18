import sys
import fileinput
import re

class Atomic():
    def __init__(self):
        self.buf = ""
        self.in_decl = ""
        self.out_decl = ""
        self.inout_decl = ""
        self.knob_decl = ""
        self.rule = ""
        self.fallback = ""

def transform(func):
    # Search for in args
    #in_args = re.search(r"__input__\[*\] (?P<name>[A-Za-z0-9_]+)(,|\n)", func.buf)
    #in_args = re.search(r"__input__\[*\] (?P<name>[A-Za-z0-9_]+)", func.buf)

    # Getting inargs
    in_arg_pattern_header = r"__input__\[(?P<size>[^\]]*)\]"
    in_arg_pattern =  in_arg_pattern_header + r" *[A-Za-z0-9_*\(\)]+ *(?P<name>[*A-Za-z0-9_]+)[,)]"
    #in_arg_pattern_header = r"__input__\[.*\]"
    #in_args = re.search(in_arg_pattern, func.buf)
    in_args = re.findall(in_arg_pattern, func.buf)
    print(in_args)
    for in_arg in in_args:
        name = in_arg[1].strip("*")
        size = in_arg[0]
        func.in_decl += "\tIN(" + name + ", " + size + ");\n"
        print(name)
        print(size)

    # Remove the inarg found
    func.buf = re.sub(in_arg_pattern_header, "", func.buf)

    # Getting outargs
    out_arg_pattern_header = r"__output__\[(?P<size>[^\]]*)\]"
    out_arg_pattern =  out_arg_pattern_header + r" *[A-Za-z0-9_*\(\)]+ *(?P<name>[*A-Za-z0-9_]+)[,)]"
    #in_arg_pattern_header = r"__input__\[.*\]"
    #in_args = re.search(in_arg_pattern, func.buf)
    out_args = re.findall(out_arg_pattern, func.buf)
    print(out_args)
    for out_arg in out_args:
        name = out_arg[1].strip("*")
        size = out_arg[0]
        func.out_decl += "\tOUT(" + name + ", " + size + ");\n"
        print(name)
        print(size)

    # Remove the inarg found
    func.buf = re.sub(out_arg_pattern_header, "", func.buf)

    # Getting inouts
    inout_arg_pattern_header = r"__inout__\[(?P<size>[^\]]*)\]"
    inout_arg_pattern =  inout_arg_pattern_header + r" *[A-Za-z0-9_*\(\)]+ *(?P<name>[*A-Za-z0-9_]+)[,)]"
    #in_arg_pattern_header = r"__input__\[.*\]"
    #in_args = re.search(in_arg_pattern, func.buf)
    inout_args = re.findall(inout_arg_pattern, func.buf)
    print(inout_args)
    for inout_arg in inout_args:
        name = inout_arg[1].strip("*")
        size = inout_arg[0]
        func.inout_decl += "\tINOUT(" + name + ", " + size + ");\n"
        print(name)
        print(size)

    # Remove the inarg found
    func.buf = re.sub(inout_arg_pattern_header, "", func.buf)

    # Getting knob vals
    knob_pattern = r"__atomic__<(?P<knobs>[^>]*)>"
    knob_match = re.search(knob_pattern, func.buf)
    if knob_match is not None:
        knobs = knob_match.group("knobs")
        knobs = knobs.split(",")
        for knob in knobs:
            if ":" in knob:
                name = knob.split(":")[0]
                default = knob.split(":")[1]
            else:
                name = knob
                default = "1"
            func.knob_decl += "\tPARAM(" + name + ", " + default + ");\n"

    # Remove knob val decl and atomic decl and instead put void
    atomic_pattern = r"__atomic__(<[^>]*>)? "
    func.buf = re.sub(atomic_pattern, "void ", func.buf)

    # Find the scaling rule
    #scaling_rule_pattern = r"__scaling_rule__ *\{(?P<rule>[^}]*(}[^{]*{)*[^}]*}?[^}]*)\} *__scaling_rule__"
    scaling_rule_pattern = r"__scaling_rule__ *\{(?P<rule>[\s\S]*)\} *__scaling_rule__"
    rule_match = re.search(scaling_rule_pattern, func.buf)
    print(rule_match)
    if rule_match is not None:
        rule = rule_match.group("rule")
        func.rule += "\tELSE();\n"
        func.rule += rule
        func.rule += "\n\tEND_IF();\n"
        func.buf = re.sub(scaling_rule_pattern, func.rule, func.buf)
    else:
        # Add END_IF(); at the very end manually
        string = "\tEND_IF();\n}"
        i = func.buf.rfind("}")
        func.buf = func.buf[:i] + string + func.buf[i+1:]


    # Find the software fallback
    # TODO: This should be more elaborated..
    #fallback_pattern = r"__fallback__ *\{(?P<rule>[^}]*(}[^{]*{)*[^}]*}?)[^}]*\} *__fallback__"
    fallback_pattern = r"__fallback__ *\{(?P<rule>[\s\S]*)\} *__fallback__"
    fallback_match = re.search(fallback_pattern, func.buf)
    if fallback_match is not None:
        fallback = fallback_match.group("rule")
        print(fallback)
        fallback = fallback.strip()
        fallback = fallback.strip(";")
        func.fallback += "\tFALLBACK(" + fallback + ");\n"
    func.buf = re.sub(fallback_pattern, "", func.buf)

    # Find the beginning and insert codes
    code = "{\n"
    code += "\tIF_ATOMIC();\n"
    code += func.fallback
    code += func.knob_decl
    code += func.in_decl
    code += func.out_decl
    code += func.inout_decl
    func.buf = re.sub(r"\{", code, func.buf, 1)

if __name__ == "__main__":
    file_name = sys.argv[1]
    new_name = file_name.split(".")[0] + "_new.c"
    f = open(new_name, "w")

    atomic_start = 0
    atomic_bracket_cnt = 0
    func = Atomic()
    new_code = ""
    for line in fileinput.input(file_name):
        # Detect atomic function start
        if re.match("__atomic__", line) is not None:
            atomic_start = 1
        # In atomic function,
        if atomic_start == 1:
            # Buffer lines
            func.buf += line

            if "{" in line:
                atomic_bracket_cnt += 1
            if "}" in line:
                atomic_bracket_cnt -= 1
                if atomic_bracket_cnt == 0:
                    # Atomic function end
                    transform(func)
                    print(func.buf)
                    f.write(func.buf)
                    func = Atomic()
                    atomic_start = 0
        else:
            # If not atomic, write directly
            f.write(line)
    f.close()


