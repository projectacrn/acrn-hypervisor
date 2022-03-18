import VConsole from 'vconsole';

const vConsole = new VConsole();
import React from 'react'
import ReactDOM from 'react-dom'

import './index.scss'
import './assets/fonts/Roboto.css'

import App from './App'
import Navbar from "./components/Navbar";
import MyErrorBoundary from "./pages/Error/MyErrorBoundary";
import {Container} from "react-bootstrap";

ReactDOM.render(
    <React.StrictMode>
        <Container fluid>
            <Navbar/>
            <MyErrorBoundary>
                <App/>
            </MyErrorBoundary>
        </Container>
    </React.StrictMode>,
    document.getElementById('root')
)
