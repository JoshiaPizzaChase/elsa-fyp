import React from 'react';
import {useNavigate} from 'react-router-dom';
import StarField from '../components/StarField';
import {useAuth} from '../context/AuthContext';
import './HomePage.css';

function HomePage() {
    const navigate = useNavigate();
    const {user} = useAuth();

    const handleStart = () => {
        navigate(user ? '/lobby' : '/login');
    };

    return (
        <div className="home-page">
            <StarField/>
            <div className="home-content">
                <h1 className="home-title">EduX</h1>
                <p className="home-slogan">The Quant Lab for Aspiring Traders</p>
            </div>
            <button className="home-button" onClick={handleStart}>
                Start Your Journey
            </button>
        </div>
    );
}

export default HomePage;
