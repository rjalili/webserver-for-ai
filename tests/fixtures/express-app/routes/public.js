const express = require('express');
const router = express.Router();
router.get('/pricing', (req, res) => res.render('pricing'));
router.use(authenticate);
router.get('/reports', (req, res) => res.render('reports'));
module.exports = router;
