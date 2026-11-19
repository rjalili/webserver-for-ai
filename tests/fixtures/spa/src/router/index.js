import { createRouter, createWebHistory } from 'vue-router';
const routes = [
  { path: '/about', component: About },
  { path: '/user/:id', component: User, meta: { requiresAuth: true },
    children: [ { path: 'edit', component: UserEdit } ] },
  { path: '/help', component: Help },
];
export default createRouter({ history: createWebHistory(), routes });
