import { getToken } from 'next-auth/jwt';
export async function middleware(req) { const t = await getToken({ req }); if (!t) return Response.redirect('/login'); }
export const config = { matcher: ['/team/:path*', '/billing-portal'] };
