import React from 'react';
import ReactDOM from 'react-dom/client';
import {BrowserRouter, Routes, Route, Navigate} from 'react-router-dom';
import App from './App';
import HomePage from './pages/HomePage';
import LobbyPage from './pages/LobbyPage';
import NavbarLayout from './components/NavbarLayout';
import LoginPage from './pages/LoginPage';

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(
    <React.StrictMode>
        <BrowserRouter>
            <Routes>
                <Route path="/" element={<HomePage/>}/>
                <Route element={<NavbarLayout/>}>
                    <Route path="/lobby" element={<LobbyPage/>}/>
                    <Route path="/login" element={<LoginPage/>}/>
                    <Route path="/:serverName/trading/:ticker" element={<App/>}/>
                </Route>
                <Route path="*" element={<Navigate to="/" replace/>}/>
            </Routes>
        </BrowserRouter>
    </React.StrictMode>
);
