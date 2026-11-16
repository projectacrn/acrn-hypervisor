import importlib.util
import io
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest import mock


MANAGER_PATH = Path(__file__).resolve().parents[1] / 'thirdLib' / 'manager.py'
SPEC = importlib.util.spec_from_file_location('configurator_thirdlib_manager', MANAGER_PATH)
manager = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(manager)


class FakeResponse:
    def __init__(self, body=b'', headers=None, error=None, raw=None):
        self.raw = raw if raw is not None else io.BytesIO(body)
        self.headers = headers or {}
        self.error = error

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        return False

    def raise_for_status(self):
        if self.error is not None:
            raise self.error


class BrokenStream:
    def __init__(self):
        self.read_count = 0

    def read(self, size=-1):
        self.read_count += 1
        if self.read_count == 1:
            return b'partial'
        raise OSError('connection interrupted')


class ManagerDownloadTests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary_directory.cleanup)
        self.destination = os.path.join(self.temporary_directory.name, 'library.whl')

    def install_download(self, response):
        with mock.patch.object(manager.requests, 'get', return_value=response) as request:
            manager.install('test-library', [{
                'type': 'download',
                'from': 'https://example.invalid/library.whl',
                'to': self.destination,
            }])
        request.assert_called_once_with(
            'https://example.invalid/library.whl', stream=True, timeout=(10, 30))

    def test_download_without_content_length_succeeds(self):
        self.install_download(FakeResponse(body=b'wheel contents'))

        with open(self.destination, 'rb') as downloaded:
            self.assertEqual(downloaded.read(), b'wheel contents')

    def test_http_error_preserves_existing_library(self):
        with open(self.destination, 'wb') as existing:
            existing.write(b'known good')

        with self.assertRaisesRegex(RuntimeError, '404'):
            self.install_download(FakeResponse(error=RuntimeError('404')))

        with open(self.destination, 'rb') as existing:
            self.assertEqual(existing.read(), b'known good')
        self.assertEqual(os.listdir(self.temporary_directory.name), ['library.whl'])

    def test_interrupted_download_does_not_publish_partial_file(self):
        with self.assertRaisesRegex(OSError, 'connection interrupted'):
            self.install_download(FakeResponse(raw=BrokenStream()))

        self.assertFalse(os.path.exists(self.destination))
        self.assertEqual(os.listdir(self.temporary_directory.name), [])


class ManagerExtractTests(unittest.TestCase):
    def test_extract_rejects_sibling_path_with_common_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            archive_path = os.path.join(directory, 'payload.tar')
            destination = os.path.join(directory, 'payload')
            os.mkdir(destination)
            with tarfile.open(archive_path, 'w') as archive:
                data = b'not allowed'
                member = tarfile.TarInfo('../payload-escape/file.txt')
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))

            with self.assertRaisesRegex(Exception, 'Path Traversal'):
                manager.install('archive', [{
                    'type': 'extract',
                    'from': archive_path,
                    'to': destination,
                }])

            self.assertFalse(os.path.exists(os.path.join(directory, 'payload-escape')))

    def test_extract_rejects_symlink_escaping_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            archive_path = os.path.join(directory, 'payload.tar')
            destination = os.path.join(directory, 'payload')
            outside = os.path.join(directory, 'outside')
            os.mkdir(destination)
            os.mkdir(outside)
            outside_file = os.path.join(outside, 'file.txt')
            with open(outside_file, 'w', encoding='utf-8') as target:
                target.write('unchanged')
            with tarfile.open(archive_path, 'w') as archive:
                link = tarfile.TarInfo('link')
                link.type = tarfile.SYMTYPE
                link.linkname = '../outside'
                archive.addfile(link)
                data = b'not allowed'
                member = tarfile.TarInfo('link/file.txt')
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))

            with self.assertRaisesRegex(Exception, 'Link Traversal'):
                manager.install('archive', [{
                    'type': 'extract',
                    'from': archive_path,
                    'to': destination,
                }])

            with open(outside_file, encoding='utf-8') as target:
                self.assertEqual(target.read(), 'unchanged')


if __name__ == '__main__':
    unittest.main()
