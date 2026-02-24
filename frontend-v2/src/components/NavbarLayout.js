import React from 'react';
import {Outlet, Link, useNavigate} from 'react-router-dom';

function NavbarLayout() {
    const navigate = useNavigate();

    return (
        <div className="app-root">
            <nav className="navbar">
                <Link to="/lobby" style={{display: 'flex', alignItems: 'center', textDecoration: 'none'}}>
                    <img src="/logo_transparent.png" alt="Logo" className="navbar-logo"/>
                    <span className="navbar-brand">EduX</span>
                </Link>
                <div style={{flex: 1}}/>
                <button className="navbar-login-btn" onClick={() => navigate('/login')}>
                    Login
                </button>
            </nav>
            <Outlet/>
        </div>
    );
}

export default NavbarLayout;
