import {ACRNProvider} from "./ACRNContext";
import {HashRouter, Routes, Route} from "react-router-dom";
import './App.css'
import Welcome from "./pages/Welcome/Welcome";
import Error from "./pages/Error/Error";
import Config from "./pages/Config/Config";

import "bootstrap/dist/js/bootstrap"

function App() {
    return (
        <ACRNProvider>
            <HashRouter>
                <Routes>
                    <Route path="/" element={<Welcome/>}/>
                    <Route path="/config" element={<Config/>}/>
                    <Route path="*" element={<Error/>}/>
                </Routes>
            </HashRouter>
        </ACRNProvider>
    )
}


export default App
