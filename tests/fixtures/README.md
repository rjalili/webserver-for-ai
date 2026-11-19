# Scanner fixtures

Small PHP, Laravel, Express, React, Vue and Next.js code samples covering the login checks
the code scanner recognises, plus decoys it must ignore (a class method named like a guard,
`app.use(session())`, an `axios.get('/api/..')` call, a webpack `output.path`).

To try them, add to a test vhost:

    AIAgentSourceDir /path/to/tests/fixtures/php-site /
    AIAgentSourceDir /path/to/tests/fixtures/express-app /app
    AIAgentSourceDir /path/to/tests/fixtures/spa /spa
    AIAgentSourceDir /path/to/tests/fixtures/laravel /

then request /.well-known/ai and compare <KnowledgeDir>/access.txt with EXPECTED_access.txt
(file paths in the evidence column differ because the php-site files live in the DocumentRoot
during the original run).
