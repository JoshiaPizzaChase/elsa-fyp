import React from 'react';
import ReactDOM from 'react-dom/client';
import {BrowserRouter, Routes, Route, Navigate} from 'react-router-dom';
import App from './App';

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(
    <React.StrictMode>
        <BrowserRouter>
            <Routes>
                <Route path="/:ticker" element={<App/>}/>
                <Route path="*" element={<Navigate to="/AAPL" replace/>}/>
            </Routes>
        </BrowserRouter>
    </React.StrictMode>
);
