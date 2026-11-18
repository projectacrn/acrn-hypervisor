import {installWebMock} from '../src/platform/webMock';
import mockData from './data/data.json';

// Kept for existing tests and consumers of the old browser setup helper.
export default function mock() {
    return installWebMock({
        files: mockData.files,
        history: mockData.history,
    });
}
