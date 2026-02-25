import React, {useState} from 'react';
import {useNavigate, Link} from 'react-router-dom';
import './AuthPage.css';

function SignUpPage() {
    const navigate = useNavigate();
    const [username, setUsername] = useState('');
    const [password, setPassword] = useState('');
    const [confirm, setConfirm] = useState('');
    const [error, setError] = useState('');
    const [loading, setLoading] = useState(false);

    const handleSubmit = async (e) => {
        e.preventDefault();
        setError('');

        if (!username.trim() || !password.trim() || !confirm.trim()) {
            setError('Please fill in all fields.');
            return;
        }
        if (password !== confirm) {
            setError('Passwords do not match.');
            return;
        }
        if (password.length < 8) {
            setError('Password must be at least 8 characters.');
            return;
        }

        setLoading(true);
        try {
            // TODO: Replace with a real DB registration call.
            //       Expected: POST /api/auth/signup { username, password }
            //       Returns: { success: boolean, error?: string }
            await new Promise((res) => setTimeout(res, 600)); // simulate latency
            const success = true; // placeholder — swap with real response check

            if (success) {
                navigate('/login');
            } else {
                setError('Username already taken. Please choose another.');
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
                <p className="auth-subtitle">Create a new account</p>

                <form className="auth-form" onSubmit={handleSubmit} noValidate>
                    <div className="auth-field">
                        <label className="auth-label" htmlFor="username">Username</label>
                        <input
                            id="username"
                            className="auth-input"
                            type="text"
                            placeholder="Choose a username"
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
                            placeholder="At least 8 characters"
                            value={password}
                            onChange={(e) => setPassword(e.target.value)}
                            autoComplete="new-password"
                        />
                    </div>

                    <div className="auth-field">
                        <label className="auth-label" htmlFor="confirm">Confirm Password</label>
                        <input
                            id="confirm"
                            className="auth-input"
                            type="password"
                            placeholder="Re-enter your password"
                            value={confirm}
                            onChange={(e) => setConfirm(e.target.value)}
                            autoComplete="new-password"
                        />
                    </div>

                    {error && <p className="auth-error">{error}</p>}

                    <button className="auth-button" type="submit" disabled={loading}>
                        {loading ? 'Creating account…' : 'Sign Up'}
                    </button>
                </form>

                <p className="auth-footer">
                    Already have an account?{' '}
                    <Link className="auth-link" to="/login">Sign in</Link>
                </p>
            </div>
        </div>
    );
}

export default SignUpPage;

