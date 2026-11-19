const router = require('express').Router();
router.get('/orders', (req, res) => res.render('orders'));
router.post('/delete-account', (req, res) => { const { reason } = req.body; res.send('gone'); });
router.get('/settings', (req, res) => res.render('settings'));
module.exports = router;
