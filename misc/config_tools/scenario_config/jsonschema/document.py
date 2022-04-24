#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#
# pyodide support status: Yes
"""
This program support run in pyodide env.
In js side you can pass params by add follow code before this script.
```js
params = "${Base64.encode(params)}"
```
"""
import re
import sys

from urllib.parse import urljoin
from subprocess import check_output

from typing import Optional

try:
    import locale  # module missing in Jython

    locale.setlocale(locale.LC_ALL, '')
except locale.Error:
    pass

from docutils.core import publish_string
from sphinx.ext.intersphinx import fetch_inventory
from bs4 import BeautifulSoup


class RSTNormalizer:
    def __init__(self, url_base, objects_inv: [dict, str]):
        self.url_base = url_base

        if isinstance(objects_inv, dict):
            # parsed data
            self.data = objects_inv
        elif isinstance(objects_inv, str):
            self.data = self.__convert_inv(objects_inv)
        else:
            raise NotImplementedError

        self.fake_roles = [
            self.fake_role('option', 'std:cmdoption'),
            self.fake_role('ref', 'std:label')
        ]

    @staticmethod
    def __convert_inv(url):
        try:
            inv_data = RSTNormalizer.__prase_inv_file(url)
        except Exception as e:
            print(e)
            print('Download inv data failed, document link will link to search page.')
            inv_data = None
        return inv_data

    @staticmethod
    def __prase_inv_file(filename):
        class MockConfig:
            intersphinx_timeout: int = None
            tls_verify = False
            user_agent = None

        class MockApp:
            srcdir = ''
            config = MockConfig()

            def warn(self, msg: str) -> None:
                print(msg, file=sys.stderr)

        invdata = fetch_inventory(MockApp(), '', filename)  # type: ignore
        result = {}
        for key in sorted(invdata or {}):
            result[key] = {}
            data = result[key]
            for entry, einfo in sorted(invdata[key].items()):
                data[entry] = {"title": einfo[3] if einfo[3] != '-' else '', "link": einfo[2]}
        return result

    def normalize(self, rest_text):
        result = re.sub(r'\.\. option::[ \t]+(\S+)', lambda x: x.group(1) + f'\n{len(x.group(1)) * "-"}', rest_text)
        for fake_role_handle in self.fake_roles:
            result = fake_role_handle(result)

        return result

    def url(self, key, data):
        title = data["title"] if data["title"] else key
        url = urljoin(self.url_base, data['link'])
        return f'`{title} <{url}>`_'

    def fake_role(self, rest_key, json_key):
        def handel_role(rest_text):
            def re_sub(match):
                key = match.group(1)
                if self.data is None or json_key not in self.data or key not in self.data[json_key]:
                    return self.url(key, {'title': '', 'link': f'search.html?q={key}&check_keywords=yes&area=default'})
                return self.url(key, self.data[json_key][key])

            return re.sub(f':{rest_key}:' + r'`(.+?)`', re_sub, rest_text)

        return handel_role


class ACRNDocumentStringConvertor:
    def __init__(self, objects_inv: Optional[dict] = None):
        self.version = self.get_acrn_document_version()
        self.url_base = 'https://projectacrn.github.io/{}/'.format(self.version)

        self.objects_inv = objects_inv
        if self.objects_inv is None:
            self.objects_inv = urljoin(self.url_base, 'objects.inv')

        self.rst_normalizer = RSTNormalizer(self.url_base, self.objects_inv)

    @staticmethod
    def get_acrn_document_version(default_version='latest'):
        version = default_version
        try:
            branch_name = check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).decode()
            version = re.match(r"^release_(\d\.\d)$", branch_name).group(1)
        except:
            print("Can't detect current acrn-hypervisor version, document string will link to latest")

        return version

    def convert(self, docstring):
        rst = self.rst_normalizer.normalize(docstring)
        html = publish_string(rst, writer_name='html5').decode('utf-8')
        soup = BeautifulSoup(html, 'lxml')
        for link in soup.select('a'):
            link['target'] = '_blank'
        try:
            fragment = soup.select_one('div.document').prettify()
        except:
            fragment = '\n'.join([str(x) for x in soup.select_one('main').children]).strip()
        return fragment


def main():
    params = {
        "text": open('configdoc.txt').read(),
        "objectsInv": None
    }
    doc_html = ACRNDocumentStringConvertor(params['objectsInv']).convert(params['text'])
    return doc_html
