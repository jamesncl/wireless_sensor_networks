#!/usr/bin/env python

import os
import argparse


def parse_args():
    parser = argparse.ArgumentParser(description="Combine frames into video")
    parser.add_argument('-f', '--framesfolder', type=str, required=False, default='video_tmp',
                        help="Path to folder containing frames. Defaults to video_tmp")
    parser.add_argument('-o', '--outfolder', type=str, required=False, default='./',
                        help="Fodler to output video file")
    return parser.parse_args()


def make_video(video_frames_folder, out_folder):

    # Combine the saved PNGs into a video
    print "Saving animation to video..."
    frame_rate = '10'  # Frames per second
    file_input_format = "'{}'".format(os.path.join(video_frames_folder, '*.png'))
    codec = 'mpeg4'
    video_output_file = os.path.join(out_folder, 'Movie.mp4')

    os.system('ffmpeg -f image2 -r {} -pattern_type glob -i {} -vcodec {} -y {} -vb 20M'.format(
        frame_rate,
        file_input_format,
        codec,
        video_output_file))
    print "Saved {}".format(video_output_file)


if __name__ == '__main__':
    args = parse_args()
    make_video(args.framesfolder, args.outfolder)


