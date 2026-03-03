import React, {useState} from 'react';
import {useNavigate, Link} from 'react-router-dom';
import {useAuth} from '../context/AuthContext';
import {login as apiLogin} from '../api';
import './AuthPage.css';

function LoginPage() {
    const navigate = useNavigate();
    const {login} = useAuth();
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const [error, setError] = useState('');
    const [loading, setLoading] = useState(false);

    const handleSubmit = async (e) => {
        e.preventDefault();
        setError('');

        if (!username.trim() || !password.trim()) {
            setError('Please enter both username and password.');
            return;
        }

        setLoading(true);
        try {
            const data = await apiLogin(username, password);
            if (data.success) {
                login(username);
                navigate('/lobby');
            } else {
                setError(data.err_msg || 'Invalid username or password.');
            }
        } catch {
            setError('Something went wrong. Please try again.');
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="auth-page">
            <div className="auth-card">
                <h1 className="auth-title">EduX</h1>
                <p className="auth-subtitle">Sign in to your account</p>

                <form className="auth-form" onSubmit={handleSubmit} noValidate>
                    <div className="auth-field">
                        <label className="auth-label" htmlFor="username">Username</label>
                        <input
                            id="username"
                            className="auth-input"
                            type="text"
                            placeholder="Enter your username"
                            value={username}
                            onChange={(e) => setUsername(e.target.value)}
                            autoComplete="username"
                            autoFocus
                        />
                    </div>

                    <div className="auth-field">
                        <label className="auth-label" htmlFor="password">Password</label>
                        <input
                            id="password"
                            className="auth-input"
                            type="password"
                            placeholder="Enter your password"
                            value={password}
                            onChange={(e) => setPassword(e.target.value)}
                            autoComplete="current-password"
                        />
                    </div>

                    {error && <p className="auth-error">{error}</p>}

                    <button className="auth-button" type="submit" disabled={loading}>
                        {loading ? 'Signing in…' : 'Sign In'}
                    </button>
                </form>

                <p className="auth-footer">
                    Don't have an account?{' '}
                    <Link className="auth-link" to="/signup">Sign up</Link>
                </p>
            </div>
        </div>
    );
}

export default LoginPage;
