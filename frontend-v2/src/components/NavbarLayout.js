import React from 'react';
import {Outlet, Link, useNavigate} from 'react-router-dom';
import {useAuth} from '../context/AuthContext';

function NavbarLayout() {
    const navigate = useNavigate();
    const {user, logout} = useAuth();

    const handleLogout = () => {
        logout();
        navigate('/');
    };

    return (
        <div className="app-root">
            <nav className="navbar">
                <Link to="/lobby" style={{display: 'flex', alignItems: 'center', textDecoration: 'none'}}>
                    <img src="/logo_transparent.png" alt="Logo" className="navbar-logo"/>
                    <span className="navbar-brand">EduX</span>
                </Link>
                <div style={{flex: 1}}/>
                {user ? (
                    <div className="navbar-user">
                        <Link to="/account" className="navbar-username">
                            {user.username}
                        </Link>
                        <button className="navbar-logout-btn" onClick={handleLogout}>
                            Logout
                        </button>
                    </div>
                ) : (
                    <button className="navbar-login-btn" onClick={() => navigate('/login')}>
                        Login
                    </button>
                )}
            </nav>
            <Outlet/>
        </div>
    );
}

export default NavbarLayout;
