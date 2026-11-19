import { Routes, Route } from 'react-router-dom';
export default function App() {
  return (
    <Routes>
      <Route path="/" element={<Home />} />
      <Route path="/login" element={<Login text="Don't have an account? Sign up" />} />
      <Route element={<RequireAuth />}>
        <Route path="/profile" element={<Profile />} />
        <Route path="billing" element={<Billing />} />
      </Route>
      <Route path="/settings" element={<ProtectedRoute><Settings /></ProtectedRoute>} />
      <Route path="/faq" element={<Faq />} />
    </Routes>
  );
}
export function Feedback() { const send = () => fetch('/api/feedback', { method: 'POST', body: JSON.stringify({ rating, comment }) }); return null; }
