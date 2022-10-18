#!/usr/bin/env python3
"""
misc/config_tools/configurator/packages/configurator/thirdLib/manager.py
depend on misc/config_tools/configurator/packages/configurator/thirdLib/library.json
"""
import argparse
import os
import json
import shutil
import tarfile
from pathlib import Path

import requests
from tqdm.auto import tqdm


def check(library_info, operation):
    if operation == 'check':
        print('Check:')
    for library in library_info['library']:
        library_name = library["name"]

        check_type = library['check']['type']
        if check_type == 'file':
            check_file = library['check']['path']
            check_result = os.path.isfile(check_file)

            if operation == 'check':
                print(f'{library["name"]}: {check_result}')
            elif operation == 'install' and not check_result:
                library_install = library["install"]
                install(library_name, library_install)
            elif operation == 'clean' and check_result:
                clean(library_name, library['clean'])


def install(library_name, library_install):
    print(f'Install: {library_name}')
    for step in library_install:
        step_type = step['type']
        if step_type == "copy":
            copy_from = step['from']
            copy_to = step['to']
            shutil.copyfile(copy_from, copy_to)
            print(f"copy: {copy_to} success")
        elif step_type == "download":
            download_from = step['from']
            download_to = step['to']

            with requests.get(download_from, stream=True) as r:
                total_length = int(r.headers.get("Content-Length"))
                with tqdm.wrapattr(r.raw, "read", total=total_length, desc="") as raw:
                    with open(download_to, 'wb') as output:
                        shutil.copyfileobj(raw, output)
            print(f'download: {download_to} success')
        elif step_type == "extract":
            tar_file: str = step['from']
            tar_mode = tar_file.split('.')[-1]
            assert tar_mode in ['gz', 'bz2', 'xz', 'tar']
            print(f'extract: {tar_file}')
            with tarfile.open(tar_file, 'r') as tar:
                def is_within_directory(directory, target):
                    
                    abs_directory = os.path.abspath(directory)
                    abs_target = os.path.abspath(target)
                
                    prefix = os.path.commonprefix([abs_directory, abs_target])
                    
                    return prefix == abs_directory
                
                def safe_extract(tar, path=".", members=None, *, numeric_owner=False):
                
                    for member in tar.getmembers():
                        member_path = os.path.join(path, member.name)
                        if not is_within_directory(path, member_path):
                            raise Exception("Attempted Path Traversal in Tar File")
                
                    tar.extractall(path, members, numeric_owner=numeric_owner) 
                    
                
                safe_extract(tar, path=step["to"], members=tar)
            print(f'extract: {tar_file} success')
        elif step_type == 'remove':
            remove_path = step['path']
            if os.path.isfile(remove_path):
                os.remove(remove_path)
            else:
                shutil.rmtree(remove_path)
            print(f'remove: {remove_path} success')
        elif step_type == "replaceText":
            filename = step['file']
            file_content = open(filename, encoding='utf-8').read()
            replace_old = step['old']
            replace_new = step['new']
            file_content = file_content.replace(replace_old, replace_new)
            open(filename, 'w', encoding='utf-8').write(file_content)
            print(f"replaceText: {filename} success")
        else:
            print(step)
            raise ValueError
    print(f'Install: {library_name} success')


def clean(library_name, library_clean):
    print(f'Clean: {library_name}')
    for clean_path in library_clean:
        if os.path.isfile(clean_path):
            os.remove(clean_path)
        elif os.path.isdir(clean_path):
            shutil.rmtree(clean_path)
        print(f'remove: {clean_path} success')
    print(f'Clean: {library_name} success')


def manager(operation, library_info):
    cwd = os.path.abspath(os.getcwd())

    third_dir = os.path.dirname(
        os.path.abspath(__file__)
    )
    os.chdir(third_dir)

    if operation in ['check', 'install', 'clean']:
        check(library_info, operation)
    os.chdir(cwd)


def main():
    library_json = Path(__file__).parent / 'library.json'

    parser = argparse.ArgumentParser(
        description='ACRN Configurator third part library manager.'
    )
    parser.add_argument('operation', choices=['check', 'install', 'clean'])
    parser.add_argument('-c', '--config', dest='config', default=library_json)

    args = parser.parse_args()
    library_info = json.load(open(args.config, encoding='utf-8'))

    manager(args.operation, library_info)


if __name__ == '__main__':
    main()
