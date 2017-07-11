'use strict;'

const expect = require('code').expect
const testRoot = require('path').resolve(__dirname, '..')
const glp = require('bindings')({ module_root: testRoot, bindings: 'glpk' })
const temp = require('temp').track()
const fs = require('fs')

glp.termOutput(false)

describe('Verify eventemitter on Problem object', function() {
    it('should have log events fired as the problem is processed asynchronously', function(done) {
        let lp = new glp.Problem()
        let idx = 0;
        let messages = [
            "GLPK Integer Optimizer",
            "0 rows, 0 columns, 0 non-zeros",
            "0 integer variables, none of which are binary",
            "Preprocessing...",
            "Objective value =   0.000000000e+00",
            "INTEGER OPTIMAL SOLUTION FOUND BY MIP PREPROCESSOR",
        ]
        lp.on('log', function(msg) {
            expect(msg).to.contain(messages[idx++])
            if(idx === messages.length) {
                done()
            }
        })
        lp.intopt({ msgLev: glp.MSG_ALL, presolve: glp.ON }, function() {
        })

    })

    it('should have log events fired as the problem is processed synchronously', function(done) {
        let lp = new glp.Problem()
        let idx = 0;
        let messages = [
            "GLPK Integer Optimizer",
            "0 rows, 0 columns, 0 non-zeros",
            "0 integer variables, none of which are binary",
            "Preprocessing...",
            "Objective value =   0.000000000e+00",
            "INTEGER OPTIMAL SOLUTION FOUND BY MIP PREPROCESSOR",
        ]
        lp.on('log', function(msg) {
            expect(msg).to.contain(messages[idx++])
        })
        lp.intoptSync({ msgLev: glp.MSG_ALL, presolve: glp.ON })
        expect(idx).to.equal(messages.length)
        done()
    })
})

describe('Verify eventemitter on mathprog', function() {
    it('should have log events fired as the prog is processed asynchronously', function(done) {
        temp.open('mathprog_test_tempfile', function(err, info) {
            fs.write(info.fd,
                    'param e := 20;\n' +
                    'set Sample := {1..2**e-1};\n' +
                    '\n' +
                    'var Mean;\n' +
                    'var E{z in Sample};\n' +
                    '\n' +
                    '/* sum of variances is zero */\n' +
                    'zumVariance: sum{z in Sample} E[z] = 0;\n' +
                    '\n' +
                    '/* Mean + variance[n] = Sample[n] */\n' +
                    'variances{z in Sample}: Mean + E[z] = z;\n' +
                    '\n' +
                    'solve;\n' +
                    '\n' +
                    'end;\n');

            let mp = new glp.Mathprog()
            let idx = 0
            let messages = [
                'Reading model section from ' + info.path,
                '15 lines were read',
            ]


            mp.on('log', function(msg) {
                expect(msg).to.contain(messages[idx++])
                if(idx === messages.length) {
                    done()
                }
            })

            mp.readModel(info.path, 0, function() {
            })
        })
    })

    it('should have log events fired as the prog is processed synchronously', function(done) {
        temp.open('mathprog_test_tempfile', function(err, info) {
            fs.write(info.fd,
                    'param e := 20;\n' +
                    'set Sample := {1..2**e-1};\n' +
                    '\n' +
                    'var Mean;\n' +
                    'var E{z in Sample};\n' +
                    '\n' +
                    'zumVariance: sum{z in Sample} E[z] = 0;\n' +
                    '\n' +
                    'variances{z in Sample}: Mean + E[z] = z;\n' +
                    '\n' +
                    'solve;\n' +
                    '\n' +
                    'end;\n');

            let mp = new glp.Mathprog()
            let idx = 0
            let messages = [
                'Reading model section from ' + info.path,
                '13 lines were read',
            ]


            mp.on('log', function(msg) {
                expect(msg).to.contain(messages[idx++])
            })

            mp.readModelSync(info.path, 0)
            expect(idx).to.equal(messages.length)
            done()
        })
    })
})

