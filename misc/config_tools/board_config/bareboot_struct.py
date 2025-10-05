from defusedxml.lxml import parse
import acrn_config_utilities

def gen_bare_boot(config):
    """
    Get Bare boot information
    """
    scenario_etree = parse(acrn_config_utilities.SCENARIO_INFO_FILE)

    print("\n#include <bare.h>", file=config)
    print("struct bare_boot_option bare_boot_options[] = {", file=config)
    for mod_element in scenario_etree.xpath(f".//BARE_BOOT_OPTIONS/MODULE"):
        print("\t{{ .addr = {0}, .size = {1}, .tag = \"{2}\" }},".format(
            mod_element.xpath(".//address/text()")[0],
            mod_element.xpath(".//size/text()")[0],
            mod_element.xpath(".//tag/text()")[0],
            ), file=config)
    print("};", file=config)
    print("uint16_t n_bare_boot_options = ARRAY_SIZE(bare_boot_options);\n", file=config)
