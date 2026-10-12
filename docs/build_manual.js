const fs = require('fs');
const {
  Document, Packer, Paragraph, TextRun, Header, Footer, PageNumber, LevelFormat, AlignmentType,
  BorderStyle, SectionType, PageBreak,
} = require('docx');
const L = require('./lib');
const { blocks } = require('./content');

const out = process.argv[3]||'COBOL-User-Guide.docx';
const pagesFile = process.argv[2]||'';
const pages = fs.existsSync(pagesFile) ? JSON.parse(fs.readFileSync(pagesFile, 'utf8')) : [];

L.assignFigures(blocks);

const PUB = 'COB-0001-0';
const HDR = 'COBOL for C89 Introduction and User’s Guide';
const A = L.ARIAL;

/* ------------------------------------------------------------ front matter */
const arialP = (text, size, bold, extra = {}) => new Paragraph({
  children: [new TextRun({ text, font: A, size, bold })], ...extra,
});
const front = [
  new Paragraph({ spacing: { before: 1800 }, children: [] }),
  arialP('COBOL', 72, true),
  arialP('A Translator from COBOL to ANSI C (C89)', 30, false, { spacing: { after: 600 } }),
  arialP('Introduction', 40, true),
  new Paragraph({
    spacing: { after: 400 },
    border: { bottom: { style: BorderStyle.SINGLE, size: 24, space: 8, color: '000000' } },
    children: [new TextRun({ text: 'and User’s Guide', font: A, size: 40, bold: true })],
  }),
  arialP('Release 1', 24, false, { spacing: { after: 120 } }),
  arialP('for VM/370 CMS and Linux', 22, false, { spacing: { after: 2400 } }),
  arialP(`Publication ${PUB}`, 22, false),
  new Paragraph({ children: [new PageBreak()] }),
  arialP('First Edition (October 2026)', 24, true, { spacing: { after: 200 } }),
  L.para('This edition applies to cobol Release 1 (October 2026), a translator from COBOL to ANSI C (C89) written in C89, and to all subsequent versions until otherwise indicated in new editions.', { after: 160 }),
  L.para('cobol is a hobbyist project. It implements a large working subset of COBOL, with some features of later dialects and object-oriented COBOL, and is not an implementation of any particular commercial product. The language is described against COBOL-85 (ANSI X3.23-1985), with selected features of COBOL 2002, 2014 and 2023.', { after: 160 }),
  L.para('IBM, COBOL (CODASYL and ISO) and GnuCOBOL are names of products or specifications of their respective owners and are used here only to identify them. Other names are trademarks of their respective owners.', { after: 160 }),
  L.para('The project repository holds the translator source, the run-time header, the test programs, the examples and this manual, and every example in the manual can be run from it. Comments on this publication may be addressed to the project.', { after: 160 }),
  new Paragraph({ children: [new PageBreak()] }),
  new Paragraph({ spacing: { after: 240 }, children: [new TextRun({ text: 'Contents', font: A, size: 32, bold: true })] }),
];

// contents list from the headings
let inPreface = false;
const tocEntries = [];
for (const b of blocks) {
  if (b.t === 'h1') { inPreface = b.text === 'Preface'; tocEntries.push({ text: b.text, level: 1 }); }
  else if (b.t === 'h2' && !inPreface) tocEntries.push({ text: b.text, level: 2 });
}
tocEntries.forEach((e, i) => front.push(L.tocLine(e.text, (pages[i] || '0'), e.level)));
fs.writeFileSync(__dirname + '/_toc.json', JSON.stringify(tocEntries));

/* ---------------------------------------------------------------- document */
const doc = new Document({
  creator: 'cobol project',
  title: 'COBOL for C89: Introduction and User’s Guide',
  description: `Publication ${PUB}`,
  styles: {
    default: { document: { run: { font: L.TNR, size: 21 } } },
    paragraphStyles: [
      {
        id: 'Heading1', name: 'Heading 1', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: A, bold: true, size: 36 },
        paragraph: {
          spacing: { before: 0, after: 360 }, outlineLevel: 0,
          border: { bottom: { style: BorderStyle.SINGLE, size: 18, space: 6, color: '000000' } },
        },
      },
      {
        id: 'Heading2', name: 'Heading 2', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: A, bold: true, size: 26 },
        paragraph: { spacing: { before: 320, after: 140 }, keepNext: true, outlineLevel: 1 },
      },
      {
        id: 'Heading3', name: 'Heading 3', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: A, bold: true, size: 22 },
        paragraph: { spacing: { before: 220, after: 100 }, keepNext: true, outlineLevel: 2 },
      },
    ],
  },
  numbering: {
    config: [
      {
        reference: 'bul',
        levels: [{ level: 0, format: LevelFormat.BULLET, text: '●', alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } } }],
      },
      {
        reference: 'num',
        levels: [{ level: 0, format: LevelFormat.DECIMAL, text: '%1.', alignment: AlignmentType.LEFT,
          style: { paragraph: { indent: { left: 720, hanging: 360 } } } }],
      },
    ],
  },
  sections: [
    {
      properties: {
        page: { size: { width: 11906, height: 16838 }, margin: { top: 1134, right: 1134, bottom: 1134, left: 1134, header: 708, footer: 708 } },
      },
      children: front,
    },
    {
      properties: {
        type: SectionType.NEXT_PAGE,
        page: {
          size: { width: 11906, height: 16838 },
          margin: { top: 1300, right: 1134, bottom: 1134, left: 1134, header: 708, footer: 708 },
          pageNumbers: { start: 1 },
        },
      },
      headers: {
        default: new Header({ children: [new Paragraph({
          border: { bottom: { style: BorderStyle.SINGLE, size: 4, space: 4, color: '000000' } },
          children: [new TextRun({ text: HDR, font: A, size: 16 })],
        })] }),
      },
      footers: {
        default: new Footer({ children: [new Paragraph({
          children: [
            new TextRun({ text: PUB, font: A, size: 16 }),
            new TextRun({ text: ' ', font: A, size: 18 }),
            new TextRun({ children: [PageNumber.CURRENT], font: A, size: 18 }),
          ],
        })] }),
      },
      children: L.render(blocks),
    },
  ],
});

Packer.toBuffer(doc).then((buf) => {
  fs.writeFileSync(out, buf);
  console.log('wrote', out, buf.length, 'bytes;', Object.keys(L.figNumbers).length, 'figures');
});
