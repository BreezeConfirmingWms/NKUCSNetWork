import express from "express";
import path from "path";
import { fileURLToPath } from 'url';
import fs from 'fs';
import bodyParser from 'body-parser';
import {JSDOM} from "jsdom";
import $ from 'jquery';


// outdated standard import library
// const express =require('express');
// const http = require('http');

const app = express();

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);


const port=3000;
app.use(express.static(path.join(__dirname, 'www')));

app.use(bodyParser.urlencoded({ extended: false }));
app.use(bodyParser.json());
app.get('/', function (req, res) {
    res.sendFile(path.join(__dirname,'www','webYumIndex.html'))
});
app.get('/refresh', (req, res) => {
    res.redirect('/');
});
app.get("/msg", (req, res) => {
    res.json({ message: "你好欢迎来到Yum的网页" });
});


app.post('/comment',(req,res) =>{
    const msg = req.body.message;
    fs.appendFile('./comments.txt',msg+'\n' , (err)=>{
        if(err)
            throw err;
        console.log('saved comment on comments.txt');
        res.redirect('/');

    })

}
    );



app.listen(port,()=>{
    console.log(`running on http://localhost:${port}`);
});