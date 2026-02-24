import React from 'react';
import {useNavigate} from 'react-router-dom';
import StarField from '../components/StarField';
import './HomePage.css';

function HomePage() {
    const navigate = useNavigate();

    return (
        <div className="home-page">
            <StarField/>
            <div className="home-content">
                <h1 className="home-title">EduX</h1>
                <p className="home-slogan">The Quant Lab for Aspiring Traders</p>
            </div>
            <button className="home-button" onClick={() => navigate('/lobby')}>
                Start Your Journey
            </button>
        </div>
    );
}

export default HomePage;

