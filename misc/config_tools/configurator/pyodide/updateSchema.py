#!/usr/bin/env python3
__package__ = 'configurator.pyodide'

from .pyodide import convert_result, nuc11_board, nuc11_scenario

import re
from lxml import etree


class GenerateSchema:

    def __init__(self, board, scenario):
        parser = etree.XMLParser(remove_blank_text=True)
        self.board_etree = etree.fromstring(board, parser)
        self.scenario = scenario

    @property
    def pcis(self):
        line = self.board_etree.xpath('/acrn-config/PCI_DEVICE/text()')[0]
        cnt = []
        for line in line.replace('\t', '').split('\n'):
            re_cpi = re.compile(r'^([0-9A-Fa-f]{1,2}:[0-1][0-9A-Fa-f]\.[0-7].*)\(')
            ret_ = re_cpi.search(line)
            if ret_:
                ret = ret_.group(1).strip()
                if re.search(r'^00:00.0', ret):  # omit 00:00.0
                    continue
                cnt.append(ret)
        return cnt

    @property
    def schemas(self):
        return self.scenario

    def update(self):
        return sorted(list(set(self.pcis) - set(self.schemas)))


def updateSchema(board, scenario):
    return convert_result(GenerateSchema(board, scenario).update())


main = updateSchema


def test():
    main(nuc11_board, nuc11_scenario)


if __name__ == '__main__':
    test()
