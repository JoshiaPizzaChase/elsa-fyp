import React, {createContext, useContext, useState} from 'react';

const AuthContext = createContext(null);

const AUTH_STORAGE_KEY = 'auth_user';

export function AuthProvider({children}) {
    // user is null when logged out, or an object like { username: 'josh' }
    // Initialise from localStorage so login survives a page refresh
    const [user, setUser] = useState(() => {
        try {
            const stored = localStorage.getItem(AUTH_STORAGE_KEY);
            return stored ? JSON.parse(stored) : null;
        } catch {
            return null;
        }
    });

    const login = (username) => {
        const userData = {username};
        localStorage.setItem(AUTH_STORAGE_KEY, JSON.stringify(userData));
        setUser(userData);
    };

    const logout = () => {
        localStorage.removeItem(AUTH_STORAGE_KEY);
        setUser(null);
    };

    return (
        <AuthContext.Provider value={{user, login, logout}}>
            {children}
        </AuthContext.Provider>
    );
}

export function useAuth() {
    return useContext(AuthContext);
}

