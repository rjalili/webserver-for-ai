const express = require('express');
const session = require('express-session');
const accountRouter = require('./routes/account');
const publicRouter = require('./routes/public');
const { requireLogin } = require('./middleware/auth');
const app = express();
app.use(session({ secret: 'x' }));          // session setup is NOT a login requirement
app.use('/account', requireLogin, accountRouter);
app.use('/', publicRouter);
app.get('/dashboard', ensureLoggedIn(), (req, res) => res.render('dash'));
app.get('/health', (req, res) => res.send('ok'));
app.get('/inbox', (req, res) => {
  if (!req.session.user) return res.redirect('/login');
  res.render('inbox');
});
axios.get('/api/not-a-route', {});         // client call, not a route
app.listen(3000);
app.post('/newsletter', (req, res) => { const { email, frequency } = req.body; res.send('ok'); });
