import React, {createContext, useContext, useState} from 'react';

const AuthContext = createContext(null);

export function AuthProvider({children}) {
    // user is null when logged out, or an object like { username: 'josh' }
    const [user, setUser] = useState(null);

    const login = (username) => setUser({username});
    const logout = () => setUser(null);

    return (
        <AuthContext.Provider value={{user, login, logout}}>
            {children}
        </AuthContext.Provider>
    );
}

export function useAuth() {
    return useContext(AuthContext);
}

